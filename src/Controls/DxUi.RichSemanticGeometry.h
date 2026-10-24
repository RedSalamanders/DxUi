#pragma once
#include "DxUi.PreparedFlowchart.h"
#include <DxUi/Configuration.h>
#include <DxUi/RichSemanticDocument.h>
#include <DxUi/RichTextLayout.h>
#include <stdexcept>

namespace DxUi::RichText::Detail
{
struct RichImageResourceSet;
// Dormant owner-thread seam for observing real uploads and injecting driver
// HRESULT failures. It is never consulted during painting/composition.
struct RichImageRasterCalls
{
    HRESULT (*createBitmap)(ID2D1RenderTarget*, D2D1_SIZE_U, const void*, UINT32, const D2D1_BITMAP_PROPERTIES&, ID2D1Bitmap**) noexcept {};
};
#if DXUI_ENABLE_DIAGNOSTICS
extern thread_local const RichImageRasterCalls* testRichImageRasterCalls;
#endif
class NativeObjectCapacity final : public std::length_error
{
public:
    using std::length_error::length_error;
};
struct SourceCaretHit
{
    std::size_t position{}, site = MarkdownNoNode;
};
// Native geometry preparation on the ControlHost owner. This service owns only
// prepared layouts and indexes; it creates no device/context/HWND/worker/timer.
// Composition never calls it. Source glyph fragments remain distinct from
// explicit source caret sites and structural editing operations.
class PreparedSemanticGeometry
{
public:
    PreparedSemanticGeometry() noexcept;
    ~PreparedSemanticGeometry() noexcept;
    PreparedSemanticGeometry(const PreparedSemanticGeometry&)            = delete;
    PreparedSemanticGeometry& operator=(const PreparedSemanticGeometry&) = delete;
    LayoutPreparationStatus Begin(IDWriteFactory* factory,
                                  std::shared_ptr<const SemanticDocument> document,
                                  const LayoutOptions& options,
                                  ID2D1Factory* drawing                              = nullptr,
                                  FlowchartResourceEpoch epochs                      = {},
                                  std::shared_ptr<const RichImageResourceSet> images = {},
                                  ID2D1RenderTarget* raster                          = nullptr);
    LayoutAdvance Advance(LayoutWorkBudget budget = {});
    void Cancel() noexcept;
    void Invalidate() noexcept;
    void Clear() noexcept;
    bool Ready() const noexcept;
    bool Pending() const noexcept;
    std::uint64_t Generation() const noexcept;
    std::uint64_t Revision() const noexcept;
    float Height() const noexcept;
    std::size_t BoxCount() const noexcept;
    bool HasNativeObjects() const noexcept;
    const SemanticDocument& Document() const;
    Hit HitTest(float x, float y) const;
    CaretGeometry Caret(std::size_t displayOffset) const;
    // Site affinity is revision-local native geometry identity, never a TSF ACP.
    // It distinguishes padded cells that share one exact raw insertion offset.
    CaretGeometry SourceCaret(std::size_t sourceOffset, std::size_t site = MarkdownNoNode) const;
    SourceCaretHit HitTestSource(float x, float y) const;
    // Nearest distinct prepared visual line, independent of inter-block gaps.
    // Retains source/site affinity and preferred x; performs no preparation.
    SourceCaretHit AdjacentSourceLine(std::size_t sourceOffset, std::size_t site, float preferredX, bool down) const;
    SourceCaretHit StepSource(std::size_t sourceOffset, std::size_t site, bool forwards) const;
    SourceCaretHit SourceLineBoundary(std::size_t sourceOffset, std::size_t site, bool end) const;
    bool IsEmptySourceSite(std::size_t sourceOffset, std::size_t site) const;
    bool IsCellSourceSite(std::size_t sourceOffset, std::size_t site) const;
    std::size_t SourceSyntaxNode(std::size_t sourceOffset, std::size_t site) const;
    std::size_t SourceSiteForSyntaxNode(std::size_t node) const;
    std::pair<std::size_t, std::size_t> VisibleSourceRange(float top, float bottom) const;
    std::vector<D2D1_RECT_F> Range(std::size_t first, std::size_t last) const;
    std::vector<D2D1_RECT_F> SourceRange(MarkdownRange source) const;
    std::optional<D2D1_RECT_F> ElementBounds(std::size_t element) const;
    std::span<const std::pair<std::size_t, std::size_t>> SourceLines() const;
    std::size_t PreviousCluster(std::size_t offset) const;
    std::size_t NextCluster(std::size_t offset) const;
    std::pair<std::size_t, std::size_t> DeletionRange(std::size_t offset, bool backwards) const;
    std::size_t LineBoundary(std::size_t offset, bool end) const;
    std::pair<std::size_t, std::size_t> VisibleRange(float top, float bottom) const;
    void Draw(ID2D1RenderTarget* target,
              ID2D1Brush* text,
              ID2D1Brush* decoration,
              D2D1_POINT_2F origin,
              float top,
              float bottom,
              ID2D1Brush* fill         = nullptr,
              float imageOpacity       = 1.0f,
              const LinkDrawing* links = nullptr) const;

private:
    struct State;
    struct Preparation;
    std::unique_ptr<State> state_;
    std::unique_ptr<Preparation> next_;
    std::uint64_t generation_{};
    bool valid_{};
    const State& Current() const;
    static std::vector<D2D1_RECT_F> RangeFromState(const State& state, std::size_t first, std::size_t last);
};
} // namespace DxUi::RichText::Detail
