#pragma once
#include <DxUi/MarkdownSyntax.h>
#include <DxUi/RichMarkdown.h>
#include <DxUi/RichTextPresentation.h>
#include <array>
#include <d2d1_1.h>
#include <dwrite.h>
#include <optional>
#include <span>
#include <utility>
#include <wil/com.h>
namespace DxUi::RichText
{
namespace Detail
{
struct PreparedLayoutAccess;
} // namespace Detail
struct LayoutOptions
{
    float widthDip             = 1.0f;
    float fontSizeDip          = 18.0f;
    float codeFontSizeDip      = 0.0f;
    float lineHeightMultiplier = 0.0f;
    float tabStopDip           = 0.0f;
    float headingScaleStep     = 0.15f;
    std::array<float, 6> headingFontSizeDip{};
    std::array<float, 6> headingLineHeightMultiplier{};
    std::wstring fontFamily                   = L"Segoe UI";
    std::wstring codeFontFamily               = L"Consolas";
    std::wstring locale                       = L"en-us"; // Empty uses DirectWrite's default, as ControlHost does.
    DWRITE_READING_DIRECTION readingDirection = DWRITE_READING_DIRECTION_LEFT_TO_RIGHT;
    DWRITE_TEXT_ALIGNMENT textAlignment       = DWRITE_TEXT_ALIGNMENT_LEADING;
    // Semantic-only minimum gaps. Null/null preserves legacy 8-DIP box gaps.
    // Either override engages blank-line credit; an unset paragraph gap is an
    // 8-DIP minimum. Real blank-source caret lines contribute to these gaps.
    std::optional<float> paragraphSpacingAfterLines; // Finite 0..4 nominal body lines.
    std::optional<float> listItemSpacingDip;         // Finite 0..256 DIP between items.
    std::optional<SemanticSpacing> semanticSpacing;
    MarkdownPresentationStyle markdownPresentation = MarkdownPresentationStyle::Default;
    bool wrap                                      = true;
    bool codeCopyAction                            = false; // Host supplies an explicit copy-intent callback.
    bool tableOverflow                             = false; // Subtle tables retain minimum widths beyond the viewport.
    // Effective device DPI for ppem-dependent native math font corrections.
    // Standalone layout callers without a device use the 96-DPI default.
    float dpi = 96.0f;
    // Independent mathematical family; a font without MATH uses em/stroke fallbacks.
    std::wstring mathFontFamily = L"Cambria Math";
    float diagramLabelWidthDip  = 0.0f;  // Zero uses 12.5 times the font size; otherwise 1..4096 DIP.
    bool autoParagraphDirection = false; // Semantic paragraphs use first-strong text with host fallback.
    float mathAbsoluteUnitScale = 1.0f;  // Semantic pt dimensions scale independently of font overrides.
    bool strikeCompletedTasks   = false; // Semantic checked-item body only; source and task marker are unchanged.
};
struct CaretGeometry
{
    float x{}, y{}, height{};
};
// Synchronous owner-thread raster attributes. The layout retains no brush or
// presentation policy after Draw. Source/geometry/navigation stay unchanged.
enum class TextInkRole : uint8_t
{
    Subtle,
    Accent,
    Link,
};
struct TextInkRange
{
    MarkdownRange display;
    TextInkRole role{};
};
struct LinkDrawing
{
    ID2D1Brush* brush             = nullptr;
    ID2D1Brush* emphasisBrush     = nullptr;
    ID2D1Brush* inactiveLinkBrush = nullptr;
    bool underline                = true;
    // Experimental renderer; canonical source coloring remains available when false.
    bool directSourceInkPreview = false;
    std::optional<MarkdownRange> hover, caret, emphasis;
    std::size_t displayOrigin{};
    // Prepared disjoint ranges, sorted by display.first. Null brushes inherit
    // ordinary ink; no style affects source offsets or text metrics.
    std::span<const TextInkRange> inkRanges;
    std::array<ID2D1Brush*, 3> inkBrushes{};
    // True selection keeps ordinary base ink beneath its selected-text pass,
    // including antialiased edges over a translucent selection fill.
    std::optional<MarkdownRange> inkExclusion;
    ID2D1Brush* markedTextFill = nullptr;
    bool outlineMarkedText     = false;
};
struct Hit
{
    std::size_t position{};
    bool trailing{};
    bool inside{}; // Actual text hit, independent of the nearest caret.
};
// Prepared text state borrows the caller's factory. It owns no HWND, graphics
// device, swap chain, timer or thread; the caller supplies preparation and scheduling services.
class PreparedLayout
{
public:
    PreparedLayout();
    ~PreparedLayout() noexcept;
    // Copies share the owner-thread DirectWrite layout. Synchronous Draw borrows
    // and restores raster attributes; source/navigation/retirement stay separate.
    PreparedLayout(const PreparedLayout& other);
    PreparedLayout& operator=(const PreparedLayout& other);
    PreparedLayout(PreparedLayout&& other);
    PreparedLayout& operator=(PreparedLayout&& other) noexcept;
    void Prepare(IDWriteFactory* factory, Projection projection, float width, float fontSize = 18.0f);
    void Prepare(IDWriteFactory* factory, Projection projection, const LayoutOptions& options);
    [[nodiscard]] HRESULT TryPrepare(IDWriteFactory* factory, Projection projection, const LayoutOptions& options);
    // Retirement is explicit and invalidates every geometry operation. No host
    // scheduling or rendering target is retained; there is no explicit factory field.
    void Clear() noexcept;
    const Projection& Content() const noexcept
    {
        return projection_;
    }
    bool Ready() const noexcept
    {
        return static_cast<bool>(state_);
    }
    float Height() const noexcept;
    std::uint64_t Generation() const noexcept
    {
        return generation_;
    }
    // Mouse and Previous/NextCluster use Unicode 17 default extended graphemes,
    // independent of the font's glyph clusters. Movement is in logical order.
    Hit HitTest(float x, float y) const;
    [[nodiscard]] HRESULT TryHitTest(float x, float y, Hit& output) const;
    // Explicit source/display ACP geometry remains available inside an element
    // for text services; it is not silently snapped to a navigation stop.
    CaretGeometry Caret(std::size_t position) const;
    [[nodiscard]] HRESULT TryCaret(std::size_t position, CaretGeometry& output) const;
    std::vector<D2D1_RECT_F> Range(std::size_t first, std::size_t last) const;
    [[nodiscard]] HRESULT TryRange(std::size_t first, std::size_t last, std::vector<D2D1_RECT_F>& output) const;
    std::size_t PreviousCluster(std::size_t position) const;
    std::size_t NextCluster(std::size_t position) const;
    [[nodiscard]] HRESULT TryPreviousCluster(std::size_t position, std::size_t& output) const;
    [[nodiscard]] HRESULT TryNextCluster(std::size_t position, std::size_t& output) const;
    // A collapsed delete at an interior ACP removes the containing grapheme;
    // an exact boundary deletes its preceding/following neighbor as requested.
    std::pair<std::size_t, std::size_t> DeletionRange(std::size_t position, bool backwards) const;
    [[nodiscard]] HRESULT TryDeletionRange(std::size_t position, bool backwards, std::pair<std::size_t, std::size_t>& output) const;
    std::size_t LineBoundary(std::size_t position, bool end) const;
    [[nodiscard]] HRESULT TryLineBoundary(std::size_t position, bool end, std::size_t& output) const;
    // Complete wrapped lines intersecting the vertical viewport, in display
    // offsets. Includes partially visible lines and works in either bidi flow.
    std::pair<std::size_t, std::size_t> VisibleRange(float top, float bottom) const;
    [[nodiscard]] HRESULT TryVisibleRange(float top, float bottom, std::pair<std::size_t, std::size_t>& output) const;
    // Draw is raster preparation, not DxUi's allocation-free composition stage.
    // Caller supplies target/brush and BeginDraw/EndDraw/clip/transform policy.
    void Draw(ID2D1RenderTarget* target, ID2D1Brush* brush, D2D1_POINT_2F origin, const LinkDrawing* links = nullptr) const;
    [[nodiscard]] HRESULT TryDraw(ID2D1RenderTarget* target, ID2D1Brush* brush, D2D1_POINT_2F origin, const LinkDrawing* links = nullptr) const;

private:
    friend class PreparedDocumentLayout;
    friend struct Detail::PreparedLayoutAccess;
    std::pair<float, float> DrawingVerticalBounds() const;
    std::pair<float, float> HorizontalTextBounds() const;
    std::span<const DWRITE_LINE_METRICS> LineMetrics() const;
    HRESULT PrepareOwned(IDWriteFactory* factory, Projection& projection, const LayoutOptions& options, float verticalOrigin = 0.0f);
    float VerticalOrigin() const noexcept;
    bool PositionedExactly() const noexcept;
    struct State;
    // Keep the returned member object stable across preparation and retirement.
    Projection projection_;
    std::unique_ptr<State> state_;
    std::uint64_t generation_{};
};
struct LayoutPreparation
{
    std::size_t preparedChunks{}, reusedChunks{}, preparedSourceUnits{};
    std::size_t wholeLayoutsPrepared{}, wholeLayoutsReused{}, wholeLayoutSourceUnits{};
};
enum class LayoutPreparationStatus
{
    Idle,
    Pending,
    Complete,
    Failed
};
struct LayoutWorkBudget
{
    std::size_t maximumLayouts    = 4u;
    std::size_t maximumBlockSteps = 4096u;
    // Image-capable semantic geometry requires at least one BGRA pixel and
    // stages large uploads in bounded tiles. Text-only/v0 services ignore this.
    std::size_t maximumUploadBytes = 4u * 1024u * 1024u;
};
struct LayoutAdvance
{
    LayoutPreparationStatus status = LayoutPreparationStatus::Idle;
    std::size_t layoutsPrepared{}, blockSteps{};
    std::size_t uploadedBytes{};
    HRESULT result = S_OK;
};
// Owner-thread, bounded v0 document geometry. Chunks end at physical source
// lines and target 1 Ki source units; 127 chunks plus one optional whole layout
// bound the retained graph. Private positioning lines preserve DirectWrite's
// document-coordinate math without entering the projection. Reuse requires the
// same vertical origin. Inexact positioning/empty suffixes retain canonical whole
// geometry; statistics report that work separately from incremental reuse.
// Source edits may retain immutable old chunks for the next Prepare. Invalidate
// rejects stale geometry; Clear additionally releases the complete retained graph.
// This service owns no HWND, graphics device, worker, timer or scheduling policy.
class PreparedDocumentLayout
{
public:
    PreparedDocumentLayout() noexcept;
    ~PreparedDocumentLayout() noexcept;
    PreparedDocumentLayout(const PreparedDocumentLayout&)            = delete;
    PreparedDocumentLayout& operator=(const PreparedDocumentLayout&) = delete;
    void Prepare(IDWriteFactory* factory, SourceSnapshot source, std::uint64_t revision, ViewMode mode, const LayoutOptions& options);
    [[nodiscard]] HRESULT TryPrepare(IDWriteFactory* factory, SourceSnapshot source, std::uint64_t revision, ViewMode mode, const LayoutOptions& options);
    HRESULT Result() const noexcept
    {
        return result_;
    }
    // Owner-thread staging. Begin coalesces the previous request only after its
    // replacement is staged successfully. Projection/index construction is still
    // one indivisible v0 operation; budgets bound advances, not CPU milliseconds.
    LayoutPreparationStatus BeginPreparation(
        IDWriteFactory* factory, SourceSnapshot source, std::uint64_t revision, ViewMode mode, const LayoutOptions& options);
    // Limits SDK layout calls and source-block/reused-placement walking separately.
    // One physical paragraph or whole compatibility layout remains indivisible.
    // Failure discards staging and preserves the published pair and generation.
    LayoutAdvance AdvancePreparation(const LayoutWorkBudget& budget = {});
    bool HasPendingPreparation() const noexcept;
    LayoutPreparation PendingPreparation() const noexcept;
    void CancelPreparation() noexcept;
    void Invalidate() noexcept;
    void Clear() noexcept;
    bool Ready() const noexcept;
    float Height() const noexcept;
    float Width() const noexcept;
    std::uint64_t Generation() const noexcept;
    std::uint64_t Revision() const noexcept;
    LayoutPreparation LastPreparation() const noexcept;
    std::size_t ChunkCount() const noexcept;
    bool UsesWholeGeometry() const noexcept;
    bool UsesWholeRaster() const noexcept;
    std::size_t DisplayLength() const;
    std::size_t SourceLength() const;
    wchar_t DisplayUnit(std::size_t position) const;
    bool SourceMatches(std::wstring_view source) const;
    // Earliest source ACP whose display mapping reaches this boundary. Used to
    // partition visual lines including their original hidden delimiters/EOLs.
    std::size_t SourceBoundary(std::size_t displayOffset) const;
    [[nodiscard]] HRESULT TrySourceBoundary(std::size_t displayOffset, std::size_t& output) const;
    std::size_t ToDisplay(std::size_t sourceOffset) const;
    [[nodiscard]] HRESULT TryToDisplay(std::size_t sourceOffset, std::size_t& output) const;
    std::size_t ToSource(std::size_t displayOffset, bool trailing = false) const;
    [[nodiscard]] HRESULT TryToSource(std::size_t displayOffset, bool trailing, std::size_t& output) const;
    std::wstring DisplayText() const;
    Hit HitTest(float x, float y) const;
    [[nodiscard]] HRESULT TryHitTest(float x, float y, Hit& output) const;
    CaretGeometry Caret(std::size_t position) const;
    [[nodiscard]] HRESULT TryCaret(std::size_t position, CaretGeometry& result) const;
    std::vector<D2D1_RECT_F> Range(std::size_t first, std::size_t last) const;
    [[nodiscard]] HRESULT TryRange(std::size_t first, std::size_t last, std::vector<D2D1_RECT_F>& output) const;
    std::size_t PreviousCluster(std::size_t position) const;
    [[nodiscard]] HRESULT TryPreviousCluster(std::size_t position, std::size_t& output) const;
    std::size_t NextCluster(std::size_t position) const;
    [[nodiscard]] HRESULT TryNextCluster(std::size_t position, std::size_t& output) const;
    std::pair<std::size_t, std::size_t> DeletionRange(std::size_t position, bool backwards) const;
    [[nodiscard]] HRESULT TryDeletionRange(std::size_t position, bool backwards, std::pair<std::size_t, std::size_t>& output) const;
    std::size_t LineBoundary(std::size_t position, bool end) const;
    [[nodiscard]] HRESULT TryLineBoundary(std::size_t position, bool end, std::size_t& output) const;
    std::pair<std::size_t, std::size_t> VisibleRange(float top, float bottom) const;
    [[nodiscard]] HRESULT TryVisibleRange(float top, float bottom, std::pair<std::size_t, std::size_t>& output) const;
    // Raster preparation only. A viewport limits draw calls to intersecting
    // chunks; composition consumes the host's existing cached surface.
    void Draw(ID2D1RenderTarget* target, ID2D1Brush* brush, D2D1_POINT_2F origin, float top, float bottom, const LinkDrawing* links = nullptr) const;
    [[nodiscard]] HRESULT TryDraw(
        ID2D1RenderTarget* target, ID2D1Brush* brush, D2D1_POINT_2F origin, float top, float bottom, const LinkDrawing* links = nullptr) const;

private:
    struct State;
    struct Preparation;
    const State& Prepared() const;
    std::size_t ChunkAt(std::size_t position) const;
    std::unique_ptr<State> state_;
    std::unique_ptr<Preparation> pending_;
    std::uint64_t generation_{};
    bool ready_{};
    HRESULT result_ = S_OK;
};

} // namespace DxUi::RichText
