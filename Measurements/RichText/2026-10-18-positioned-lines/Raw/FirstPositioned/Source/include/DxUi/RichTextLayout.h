#pragma once
#include <DxUi/RichMarkdown.h>
#include <d2d1_1.h>
#include <dwrite.h>
#include <span>
#include <utility>
#include <wil/com.h>
namespace DxUi::RichText
{
struct LayoutOptions
{
    float widthDip                            = 1.0f;
    float fontSizeDip                         = 18.0f;
    std::wstring fontFamily                   = L"Segoe UI";
    std::wstring codeFontFamily               = L"Consolas";
    std::wstring locale                       = L"en-us"; // Empty uses DirectWrite's default, as ControlHost does.
    DWRITE_READING_DIRECTION readingDirection = DWRITE_READING_DIRECTION_LEFT_TO_RIGHT;
};
struct CaretGeometry
{
    float x{}, y{}, height{};
};
struct Hit
{
    std::size_t position{};
    bool trailing{};
};
// Prepared text state borrows the caller's factory. It owns no HWND, graphics
// device, swap chain, timer or thread; the caller supplies preparation and scheduling services.
class PreparedLayout
{
public:
    PreparedLayout();
    ~PreparedLayout() noexcept;
    // Copies share the immutable DirectWrite layout on its preparation owner;
    // their source, navigation indexes and retirement storage remain separate.
    PreparedLayout(const PreparedLayout& other);
    PreparedLayout& operator=(const PreparedLayout& other);
    PreparedLayout(PreparedLayout&& other);
    PreparedLayout& operator=(PreparedLayout&& other) noexcept;
    void Prepare(IDWriteFactory* factory, Projection projection, float width, float fontSize = 18.0f);
    void Prepare(IDWriteFactory* factory, Projection projection, const LayoutOptions& options);
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
    // Explicit source/display ACP geometry remains available inside an element
    // for text services; it is not silently snapped to a navigation stop.
    CaretGeometry Caret(std::size_t position) const;
    std::vector<D2D1_RECT_F> Range(std::size_t first, std::size_t last) const;
    std::size_t PreviousCluster(std::size_t position) const;
    std::size_t NextCluster(std::size_t position) const;
    // A collapsed delete at an interior ACP removes the containing grapheme;
    // an exact boundary deletes its preceding/following neighbor as requested.
    std::pair<std::size_t, std::size_t> DeletionRange(std::size_t position, bool backwards) const;
    std::size_t LineBoundary(std::size_t position, bool end) const;
    // Complete wrapped lines intersecting the vertical viewport, in display
    // offsets. Includes partially visible lines and works in either bidi flow.
    std::pair<std::size_t, std::size_t> VisibleRange(float top, float bottom) const;
    // Draw is raster preparation, not DxUi's allocation-free composition stage.
    // Caller supplies target/brush and BeginDraw/EndDraw/clip/transform policy.
    void Draw(ID2D1RenderTarget* target, ID2D1Brush* brush, D2D1_POINT_2F origin) const;

private:
    friend class PreparedDocumentLayout;
    std::pair<float, float> DrawingVerticalBounds() const;
    std::span<const DWRITE_LINE_METRICS> LineMetrics() const;
    void PrepareOwned(IDWriteFactory* factory, Projection& projection, const LayoutOptions& options, float verticalOrigin = 0.0f);
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
// Owner-thread, bounded v0 document geometry. Chunks end at physical source
// lines and target 1 Ki source units; 127 chunks plus one optional whole layout
// bound the retained graph. Rounding-sensitive geometry/raster uses the canonical
// whole layout; statistics report that work separately from incremental reuse.
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
    void Invalidate() noexcept;
    void Clear() noexcept;
    bool Ready() const noexcept;
    float Height() const noexcept;
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
    std::size_t ToDisplay(std::size_t sourceOffset) const;
    std::size_t ToSource(std::size_t displayOffset, bool trailing = false) const;
    std::wstring DisplayText() const;
    Hit HitTest(float x, float y) const;
    CaretGeometry Caret(std::size_t position) const;
    std::vector<D2D1_RECT_F> Range(std::size_t first, std::size_t last) const;
    std::size_t PreviousCluster(std::size_t position) const;
    std::size_t NextCluster(std::size_t position) const;
    std::pair<std::size_t, std::size_t> DeletionRange(std::size_t position, bool backwards) const;
    std::size_t LineBoundary(std::size_t position, bool end) const;
    std::pair<std::size_t, std::size_t> VisibleRange(float top, float bottom) const;
    // Raster preparation only. A viewport limits draw calls to intersecting
    // chunks; composition consumes the host's existing cached surface.
    void Draw(ID2D1RenderTarget* target, ID2D1Brush* brush, D2D1_POINT_2F origin, float top, float bottom) const;

private:
    struct State;
    const State& Prepared() const;
    std::size_t ChunkAt(std::size_t position) const;
    std::unique_ptr<State> state_;
    std::uint64_t generation_{};
    bool ready_{};
};

} // namespace DxUi::RichText
