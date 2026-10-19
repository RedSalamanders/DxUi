#pragma once
#include <DxUi/RichSemanticDocument.h>
#include <DxUi/RichTextLayout.h>

namespace DxUi::RichText::Detail
{
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
    LayoutPreparationStatus Begin(IDWriteFactory* factory, std::shared_ptr<const SemanticDocument> document, const LayoutOptions& options);
    LayoutAdvance Advance(LayoutWorkBudget budget = {});
    void Cancel() noexcept;
    void Invalidate() noexcept;
    void Clear() noexcept;
    bool Ready() const noexcept;
    bool Pending() const noexcept;
    std::uint64_t Generation() const noexcept;
    std::uint64_t Revision() const noexcept;
    float Height() const noexcept;
    const SemanticDocument& Document() const;
    Hit HitTest(float x, float y) const;
    CaretGeometry Caret(std::size_t displayOffset) const;
    std::vector<D2D1_RECT_F> Range(std::size_t first, std::size_t last) const;
    std::vector<D2D1_RECT_F> SourceRange(MarkdownRange source) const;
    std::optional<D2D1_RECT_F> ElementBounds(std::size_t element) const;
    std::span<const std::pair<std::size_t, std::size_t>> SourceLines() const;
    std::size_t PreviousCluster(std::size_t offset) const;
    std::size_t NextCluster(std::size_t offset) const;
    std::pair<std::size_t, std::size_t> DeletionRange(std::size_t offset, bool backwards) const;
    std::size_t LineBoundary(std::size_t offset, bool end) const;
    std::pair<std::size_t, std::size_t> VisibleRange(float top, float bottom) const;
    void Draw(ID2D1RenderTarget* target, ID2D1Brush* text, ID2D1Brush* decoration, D2D1_POINT_2F origin, float top, float bottom) const;

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
