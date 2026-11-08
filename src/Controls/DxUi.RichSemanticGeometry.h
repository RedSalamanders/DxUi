#pragma once
#include "DxUi.PreparedFlowchart.h"
#include "DxUi.SemanticExtensions.h"
#include <DxUi/Configuration.h>
#include <DxUi/RichSemanticDocument.h>
#include <DxUi/RichTextLayout.h>
#include <span>
#include <stdexcept>
#include <vector>

namespace DxUi::RichText::Detail
{
// The visual and input routes share this hit rectangle. Narrow code frames
// omit the action so it never covers editable text.
inline std::optional<D2D1_RECT_F> CodeCopyBounds(D2D1_RECT_F frame, bool rtl) noexcept
{
    if (frame.right - frame.left < 96.0f || frame.bottom - frame.top < 20.0f)
        return std::nullopt;
    const auto left = rtl ? frame.left + 4.0f : frame.right - 22.0f;
    return D2D1_RECT_F{left, frame.top + 3.0f, left + 18.0f, frame.top + 21.0f};
}
struct RichImageResourceSet;
struct NativeObjectRejection
{
    std::size_t element{};
    NativeFallbackReason reason{};
};
// Dormant owner-thread seam for observing real uploads and injecting driver
// HRESULT failures. It is never consulted during painting/composition.
struct RichImageRasterCalls
{
    HRESULT (*createBitmap)(ID2D1RenderTarget*, D2D1_SIZE_U, const void*, UINT32, const D2D1_BITMAP_PROPERTIES&, ID2D1Bitmap**) noexcept {};
    HRESULT (*copyBitmap)(ID2D1Bitmap*, const D2D1_RECT_U&, const void*, UINT32) noexcept {};
};
#if DXUI_ENABLE_DIAGNOSTICS
extern thread_local const RichImageRasterCalls* testRichImageRasterCalls;
extern thread_local bool testRichSvgTargetUnsupported;
extern thread_local std::size_t testRichSvgRasterizations;
// Private owner-thread seams for proving that native preparation errors retain
// their HRESULT and that the editor can substitute an instrumented factory.
extern thread_local IDWriteFactory* testRichSemanticWriteFactory;
// 1: semantic staging, 2: native geometry; persistent until the test clears it.
extern thread_local bool testRichScriptTargetUnsupported;
extern thread_local std::size_t testRichScriptColorTargetQueries;
extern thread_local HRESULT testRichScriptColorTargetFailure;
extern thread_local unsigned testRichPreparationFailure;
extern thread_local std::size_t testRichPreparationFailureAttempts;
#endif
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
    HRESULT Result() const noexcept
    {
        return beginResult_;
    }
    std::span<const NativeObjectRejection> RejectedNativeObjects() const noexcept;
    void Cancel() noexcept;
    void Invalidate() noexcept;
    void Clear() noexcept;
    bool Ready() const noexcept;
    bool Pending() const noexcept;
    std::uint64_t Generation() const noexcept;
    std::uint64_t Revision() const noexcept;
    float Height() const noexcept;
    float Width() const noexcept;
    bool HasMarkedText() const noexcept;
    std::size_t BoxCount() const noexcept;
    std::size_t ReusedBoxCount() const noexcept;
    bool HasNativeObjects() const noexcept;
    const SemanticDocument& Document() const;
    Hit HitTest(float x, float y) const;
    [[nodiscard]] HRESULT TryHitTest(float x, float y, Hit& output) const;
    CaretGeometry Caret(std::size_t displayOffset) const;
    [[nodiscard]] HRESULT TryCaret(std::size_t displayOffset, CaretGeometry& output) const;
    // Site affinity is revision-local native geometry identity, never a TSF ACP.
    // It distinguishes padded cells that share one exact raw insertion offset.
    CaretGeometry SourceCaret(std::size_t sourceOffset, std::size_t site = MarkdownNoNode) const;
    [[nodiscard]] HRESULT TrySourceCaret(std::size_t sourceOffset, std::size_t site, CaretGeometry& output) const;
    SourceCaretHit HitTestSource(float x, float y) const;
    [[nodiscard]] HRESULT TryHitTestSource(float x, float y, SourceCaretHit& output) const;
    // Nearest distinct prepared visual line, independent of inter-block gaps.
    // Retains source/site affinity and preferred x; performs no preparation.
    SourceCaretHit AdjacentSourceLine(std::size_t sourceOffset, std::size_t site, float preferredX, bool down) const;
    [[nodiscard]] HRESULT TryAdjacentSourceLine(std::size_t sourceOffset, std::size_t site, float preferredX, bool down, SourceCaretHit& output) const;
    SourceCaretHit StepSource(std::size_t sourceOffset, std::size_t site, bool forwards) const;
    [[nodiscard]] HRESULT TryStepSource(std::size_t sourceOffset, std::size_t site, bool forwards, SourceCaretHit& output) const;
    SourceCaretHit SourceLineBoundary(std::size_t sourceOffset, std::size_t site, bool end) const;
    [[nodiscard]] HRESULT TrySourceLineBoundary(std::size_t sourceOffset, std::size_t site, bool end, SourceCaretHit& output) const;
    bool IsEmptySourceSite(std::size_t sourceOffset, std::size_t site) const;
    bool IsCellSourceSite(std::size_t sourceOffset, std::size_t site) const;
    std::size_t SourceSyntaxNode(std::size_t sourceOffset, std::size_t site) const;
    std::size_t SourceSiteForSyntaxNode(std::size_t node) const;
    std::pair<std::size_t, std::size_t> VisibleSourceRange(float top, float bottom) const;
    [[nodiscard]] HRESULT TryVisibleSourceRange(float top, float bottom, std::pair<std::size_t, std::size_t>& output) const;
    std::vector<D2D1_RECT_F> Range(std::size_t first, std::size_t last) const;
    [[nodiscard]] HRESULT TryRange(std::size_t first, std::size_t last, std::vector<D2D1_RECT_F>& output) const;
    std::vector<D2D1_RECT_F> SourceRange(MarkdownRange source) const;
    [[nodiscard]] HRESULT TrySourceRange(MarkdownRange source, std::vector<D2D1_RECT_F>& output) const;
    std::optional<D2D1_RECT_F> ElementBounds(std::size_t element) const;
    [[nodiscard]] HRESULT TryElementBounds(std::size_t element, std::optional<D2D1_RECT_F>& output) const;
    std::vector<std::size_t> VisibleCodeElements(float top, float bottom) const;
    [[nodiscard]] HRESULT TryVisibleCodeElements(float top, float bottom, std::vector<std::size_t>& output) const;
    std::span<const std::pair<std::size_t, std::size_t>> SourceLines() const;
    std::size_t PreviousCluster(std::size_t offset) const;
    [[nodiscard]] HRESULT TryPreviousCluster(std::size_t offset, std::size_t& output) const;
    std::size_t NextCluster(std::size_t offset) const;
    [[nodiscard]] HRESULT TryNextCluster(std::size_t offset, std::size_t& output) const;
    std::pair<std::size_t, std::size_t> DeletionRange(std::size_t offset, bool backwards) const;
    [[nodiscard]] HRESULT TryDeletionRange(std::size_t offset, bool backwards, std::pair<std::size_t, std::size_t>& output) const;
    std::size_t LineBoundary(std::size_t offset, bool end) const;
    [[nodiscard]] HRESULT TryLineBoundary(std::size_t offset, bool end, std::size_t& output) const;
    std::pair<std::size_t, std::size_t> VisibleRange(float top, float bottom) const;
    [[nodiscard]] HRESULT TryVisibleRange(float top, float bottom, std::pair<std::size_t, std::size_t>& output) const;
    // The retained editor opts into per-box content ink for diagram structure;
    // explicit snapshot/export callers retain their decoration/stroke contract.
    void Draw(ID2D1RenderTarget* target,
              ID2D1Brush* text,
              ID2D1Brush* decoration,
              D2D1_POINT_2F origin,
              float top,
              float bottom,
              ID2D1Brush* fill                         = nullptr,
              float imageOpacity                       = 1.0f,
              const LinkDrawing* links                 = nullptr,
              ID2D1Brush* quoteFill                    = nullptr,
              const DiagramColorContext* diagramColors = nullptr,
              ID2D1Brush* calloutAccent                = nullptr,
              ID2D1Brush* propertiesInk                = nullptr,
              bool diagramContentInk                   = false,
              ID2D1Brush* diagramLabelFill             = nullptr) const;
    [[nodiscard]] HRESULT TryDraw(ID2D1RenderTarget* target,
                                  ID2D1Brush* text,
                                  ID2D1Brush* decoration,
                                  D2D1_POINT_2F origin,
                                  float top,
                                  float bottom,
                                  ID2D1Brush* fill                         = nullptr,
                                  float imageOpacity                       = 1.0f,
                                  const LinkDrawing* links                 = nullptr,
                                  ID2D1Brush* quoteFill                    = nullptr,
                                  const DiagramColorContext* diagramColors = nullptr,
                                  ID2D1Brush* calloutAccent                = nullptr,
                                  ID2D1Brush* propertiesInk                = nullptr,
                                  bool diagramContentInk                   = false,
                                  ID2D1Brush* diagramLabelFill             = nullptr) const;

private:
    struct State;
    struct Preparation;
    std::unique_ptr<State> state_;
    std::unique_ptr<Preparation> next_;
    std::vector<NativeObjectRejection> rejectedNativeObjects_;
    std::uint64_t generation_{};
    bool valid_{};
    HRESULT beginResult_ = S_OK;
    const State& Current() const;
    static HRESULT TryRangeFromState(const State& state, std::size_t first, std::size_t last, std::vector<D2D1_RECT_F>& output);
};
} // namespace DxUi::RichText::Detail
