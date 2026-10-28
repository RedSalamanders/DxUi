#pragma once
#include "DxUi.RichSemanticGeometry.h"

namespace DxUi::RichText::Detail
{
// The retained control keeps its v0 behavior until semantic rendering is
// explicitly enabled. Each target stages its complete graph before switching;
// a new source request cannot mix one map with the other engine's geometry.
// Existing identity/RAII/reference traits intentionally delete these implicit operations.
// Preserve aggregate and overload behavior. Owner: DxUi maintainers; review: 2027-01-31.
#pragma warning(push)
#pragma warning(disable : 4625 4626 5026 5027)
class RichEditorLayout
{
public:
    LayoutPreparationStatus BeginPreparation(IDWriteFactory* factory,
                                             SourceSnapshot source,
                                             std::uint64_t revision,
                                             ViewMode mode,
                                             const LayoutOptions& options,
                                             std::shared_ptr<const SemanticDocument> document   = {},
                                             ID2D1Factory* drawing                              = nullptr,
                                             FlowchartResourceEpoch epochs                      = {},
                                             std::shared_ptr<const RichImageResourceSet> images = {},
                                             ID2D1RenderTarget* raster                          = nullptr)
    {
        const bool semantic = mode == ViewMode::Rich && static_cast<bool>(document);
        const auto result   = semantic ? semantic_.Begin(factory, std::move(document), options, drawing, epochs, std::move(images), raster)
                                       : v0_.BeginPreparation(factory, std::move(source), revision, mode, options);
        requestedSemantic_  = semantic;
        if (semantic)
            v0_.CancelPreparation();
        else
            semantic_.Cancel();
        if (result == LayoutPreparationStatus::Complete)
            Publish();
        return result;
    }
    LayoutAdvance AdvancePreparation(LayoutWorkBudget budget = {})
    {
        auto result = requestedSemantic_ ? semantic_.Advance(budget) : v0_.AdvancePreparation(budget);
        if (result.status == LayoutPreparationStatus::Complete)
            Publish();
        return result;
    }
    void CancelPreparation() noexcept
    {
        v0_.CancelPreparation();
        semantic_.Cancel();
    }
    void Invalidate() noexcept
    {
        v0_.Invalidate();
        semantic_.Invalidate();
    }
    void Clear() noexcept
    {
        v0_.Clear();
        semantic_.Clear();
        activeSemantic_ = requestedSemantic_ = false;
        hasPublication_                      = false;
        ++generation_;
    }
    bool Ready() const noexcept
    {
        return activeSemantic_ ? semantic_.Ready() : v0_.Ready();
    }
    bool HasPendingPreparation() const noexcept
    {
        return requestedSemantic_ ? semantic_.Pending() : v0_.HasPendingPreparation();
    }
    std::uint64_t Revision() const noexcept
    {
        return activeSemantic_ ? semantic_.Revision() : v0_.Revision();
    }
    std::uint64_t Generation() const noexcept
    {
        return generation_;
    }
    float Height() const noexcept
    {
        return activeSemantic_ ? semantic_.Height() : v0_.Height();
    }
    std::size_t SourceLength() const
    {
        return activeSemantic_ ? semantic_.Document().Map().Source().size() : v0_.SourceLength();
    }
    std::size_t DisplayLength() const
    {
        return activeSemantic_ ? semantic_.Document().Map().Display().size() : v0_.DisplayLength();
    }
    wchar_t DisplayUnit(std::size_t index) const
    {
        return activeSemantic_ ? semantic_.Document().Map().Display().at(index) : v0_.DisplayUnit(index);
    }
    bool SourceMatches(std::wstring_view source) const
    {
        return activeSemantic_ ? semantic_.Document().Map().Source() == source : v0_.SourceMatches(source);
    }
    std::size_t ToDisplay(std::size_t source) const
    {
        return activeSemantic_ ? semantic_.Document().Map().ToDisplay(source) : v0_.ToDisplay(source);
    }
    std::size_t ToSource(std::size_t display, bool trailing = false) const
    {
        return activeSemantic_ ? semantic_.Document().Map().ToSource(display, trailing ? MappingAffinity::Trailing : MappingAffinity::Leading)
                               : v0_.ToSource(display, trailing);
    }
    std::size_t SourceBoundary(std::size_t display) const
    {
        return activeSemantic_ ? semantic_.Document().Map().ToSource(display) : v0_.SourceBoundary(display);
    }
    std::wstring DisplayText() const
    {
        return activeSemantic_ ? std::wstring(semantic_.Document().Map().Display()) : v0_.DisplayText();
    }
    std::size_t ChunkCount() const noexcept
    {
        return activeSemantic_ ? semantic_.BoxCount() : v0_.ChunkCount();
    }
    LayoutPreparation LastPreparation() const noexcept
    {
        return activeSemantic_ && semantic_.Ready() ? LayoutPreparation{semantic_.BoxCount(), 0u, SourceLength(), 0u, 0u, 0u} : v0_.LastPreparation();
    }
    bool UsesWholeGeometry() const noexcept
    {
        return ! activeSemantic_ && v0_.UsesWholeGeometry();
    }
    bool UsesWholeRaster() const noexcept
    {
        return ! activeSemantic_ && v0_.UsesWholeRaster();
    }
    Hit HitTest(float x, float y) const
    {
        return activeSemantic_ ? semantic_.HitTest(x, y) : v0_.HitTest(x, y);
    }
    CaretGeometry Caret(std::size_t display) const
    {
        return activeSemantic_ ? semantic_.Caret(display) : v0_.Caret(display);
    }
    CaretGeometry SourceCaret(std::size_t source, std::size_t site = MarkdownNoNode) const
    {
        return activeSemantic_ ? semantic_.SourceCaret(source, site) : v0_.Caret(v0_.ToDisplay(source));
    }
    SourceCaretHit HitTestSource(float x, float y) const
    {
        if (activeSemantic_)
            return semantic_.HitTestSource(x, y);
        const auto hit = v0_.HitTest(x, y);
        return {v0_.ToSource(hit.position, hit.trailing)};
    }
    std::pair<std::size_t, std::size_t> VisibleSourceRange(float top, float bottom) const
    {
        if (activeSemantic_)
            return semantic_.VisibleSourceRange(top, bottom);
        const auto range = v0_.VisibleRange(top, bottom);
        return {v0_.ToSource(range.first), v0_.ToSource(range.second, true)};
    }
    std::vector<D2D1_RECT_F> Range(std::size_t first, std::size_t last) const
    {
        return activeSemantic_ ? semantic_.Range(first, last) : v0_.Range(first, last);
    }
    std::vector<D2D1_RECT_F> SourceRange(std::size_t first, std::size_t last) const
    {
        return activeSemantic_ ? semantic_.SourceRange({first, last}) : v0_.Range(v0_.ToDisplay(first), v0_.ToDisplay(last));
    }
    std::size_t PreviousCluster(std::size_t display) const
    {
        return activeSemantic_ ? semantic_.PreviousCluster(display) : v0_.PreviousCluster(display);
    }
    std::size_t NextCluster(std::size_t display) const
    {
        return activeSemantic_ ? semantic_.NextCluster(display) : v0_.NextCluster(display);
    }
    std::pair<std::size_t, std::size_t> DeletionRange(std::size_t display, bool backwards) const
    {
        return activeSemantic_ ? semantic_.DeletionRange(display, backwards) : v0_.DeletionRange(display, backwards);
    }
    std::size_t LineBoundary(std::size_t display, bool end) const
    {
        return activeSemantic_ ? semantic_.LineBoundary(display, end) : v0_.LineBoundary(display, end);
    }
    std::pair<std::size_t, std::size_t> VisibleRange(float top, float bottom) const
    {
        return activeSemantic_ ? semantic_.VisibleRange(top, bottom) : v0_.VisibleRange(top, bottom);
    }
    void Draw(ID2D1RenderTarget* target,
              ID2D1Brush* brush,
              ID2D1Brush* decoration,
              D2D1_POINT_2F origin,
              float top,
              float bottom,
              ID2D1Brush* fill         = nullptr,
              float imageOpacity       = 1.0f,
              const LinkDrawing* links = nullptr,
              ID2D1Brush* quoteFill    = nullptr) const
    {
        if (activeSemantic_)
            semantic_.Draw(target, brush, decoration, origin, top, bottom, fill, imageOpacity, links, quoteFill);
        else
            v0_.Draw(target, brush, origin, top, bottom, links);
    }
    bool Semantic() const noexcept
    {
        return activeSemantic_;
    }
    const PreparedSemanticGeometry* SemanticGeometry() const noexcept
    {
        return activeSemantic_ && semantic_.Ready() ? &semantic_ : nullptr;
    }

private:
    void Publish() noexcept
    {
        const auto targetGeneration = requestedSemantic_ ? semantic_.Generation() : v0_.Generation();
        if (! hasPublication_ || activeSemantic_ != requestedSemantic_ || targetGeneration != targetGeneration_)
            ++generation_;
        hasPublication_   = true;
        activeSemantic_   = requestedSemantic_;
        targetGeneration_ = targetGeneration;
        if (activeSemantic_)
            v0_.Clear();
        else
            semantic_.Clear();
    }
    PreparedDocumentLayout v0_;
    PreparedSemanticGeometry semantic_;
    std::uint64_t generation_{}, targetGeneration_{};
    bool activeSemantic_{}, requestedSemantic_{}, hasPublication_{};
};
#pragma warning(pop)
} // namespace DxUi::RichText::Detail
