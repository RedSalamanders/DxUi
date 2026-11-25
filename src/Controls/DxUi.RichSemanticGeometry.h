#pragma once
#include "DxUi.PreparedFlowchart.h"
#include "DxUi.SemanticExtensions.h"
#include <DxUi/Configuration.h>
#include <DxUi/RichSemanticDocument.h>
#include <DxUi/RichTextLayout.h>
#include <cstdint>
#include <optional>
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
// The strips an SVG raster is drawn in, a few at each owner turn (W7.7). Each strip walks the whole document tree and draws
// the pixels of its clip, so the count follows from an estimate of both: a strip holds at most what one turn's upload budget
// holds (its bytes at four per pixel), less what walking the document's elements costs (taken as 64 pixels an element), and
// never less than half of that budget. Full-width rows of at least one, or columns of one row when a row is wider than a
// strip. `elements` is SvgElementEstimate of the document. Pure and deterministic.
struct SvgStripPlan
{
    std::uint32_t columns{}, rows{};
};
[[nodiscard]] SvgStripPlan PlanSvgStrips(std::uint32_t width, std::uint32_t height, std::size_t elements, std::size_t turnBytes) noexcept;
// The elements of an SVG document, counted without parsing it: each `<` that opens a name.
[[nodiscard]] std::size_t SvgElementEstimate(std::span<const std::byte> document) noexcept;
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
// W2.3 negative control: while set, a formula carries without comparing its source (a deliberately wrong key), so the
// incremental-versus-fresh oracle and the geometry tests must report a difference after an edit inside a formula.
extern thread_local bool testRichMathCarryIgnoresSource;
#endif
struct SourceCaretHit
{
    std::size_t position{}, site = MarkdownNoNode;
};
// Pointer-hover lookup by display offset, built at preparation with each geometry generation. The boundaries of the
// indexed spans cut the display into segments, and each segment records the element a hover there resolves to, so a
// lookup is one binary search over the boundaries and examines no element.
enum class HoverSpanPick : std::uint8_t
{
    Smallest, // The shortest containing span; on equal length the later element.
    First     // The containing span of the lowest element index.
};
struct HoverSpan
{
    std::size_t first{}, last{}, element{};
};
class HoverSpanIndex
{
public:
    // Replaces the index. An empty span (first >= last) contains no offset and is ignored. Overlapping spans are allowed.
    void Build(std::span<const HoverSpan> spans, HoverSpanPick pick);
    [[nodiscard]] std::optional<std::size_t> At(std::size_t display) const noexcept;
    [[nodiscard]] std::size_t SegmentCount() const noexcept
    {
        return starts_.size();
    }

private:
    std::vector<std::size_t> starts_ = std::vector<std::size_t>(0u), elements_ = std::vector<std::size_t>(0u);
};
// Native geometry preparation on the ControlHost owner. This service owns only
// prepared layouts and indexes; it creates no device/context/HWND/worker/timer.
// Composition never calls it. Source glyph fragments remain distinct from
// explicit source caret sites and structural editing operations.
class CodeLayoutCache;
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
    // The hover index of this generation: the titled link, image or abbreviation whose title a hover at a display offset
    // shows (the smallest such span, the later element on equal length; an expanded source owner excluded), and the link,
    // footnote reference or backlink it underlines (the first in element order).
    std::optional<std::size_t> HoverTitleElement(std::size_t displayOffset) const;
    std::optional<std::size_t> HoverLinkElement(std::size_t displayOffset) const;
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
    // The two visible-range queries above find the boxes meeting the window through the paint index (W7.6). These two
    // visit every box in order instead, as the queries did before: the reference the tests compare them with.
    [[nodiscard]] HRESULT TryVisibleRangeByFullScan(float top, float bottom, std::pair<std::size_t, std::size_t>& output) const;
    [[nodiscard]] HRESULT TryVisibleSourceRangeByFullScan(float top, float bottom, std::pair<std::size_t, std::size_t>& output) const;
    // The line number layouts of the published geometry's numbered code fences, for tests: how many, the layout and the
    // origin each is drawn at (W7.6), and what the layout cache of line numbers and badges holds (W7.6).
    std::size_t CodeNumberCount() const noexcept;
    const PreparedLayout& CodeNumberLayout(std::size_t index) const;
    D2D1_POINT_2F CodeNumberOrigin(std::size_t index) const;
    std::size_t CodeLayoutCacheEntries() const noexcept;
    std::uint64_t CodeLayoutCacheBytes() const noexcept;
    void ReleaseCodeLayoutCache() noexcept;
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
    std::unique_ptr<CodeLayoutCache> codeLayouts_; // W7.6; registered as a retained cache (DxUi.RetainedCaches.cpp)
    [[nodiscard]] HRESULT VisibleRangeOf(float top, float bottom, std::pair<std::size_t, std::size_t>& output, bool fullScan) const;
    [[nodiscard]] HRESULT VisibleSourceRangeOf(float top, float bottom, std::pair<std::size_t, std::size_t>& output, bool fullScan) const;
    std::vector<NativeObjectRejection> rejectedNativeObjects_;
    std::uint64_t generation_{};
    bool valid_{};
    HRESULT beginResult_ = S_OK;
    const State& Current() const;
    static HRESULT TryRangeFromState(const State& state, std::size_t first, std::size_t last, std::vector<D2D1_RECT_F>& output);
};
} // namespace DxUi::RichText::Detail
