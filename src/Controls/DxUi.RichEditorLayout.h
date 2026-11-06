#pragma once
#include "DxUi.RichSemanticGeometry.h"

namespace DxUi::RichText::Detail
{
#if DXUI_ENABLE_DIAGNOSTICS
inline thread_local std::size_t testRichLayoutDrawCalls{};
#endif
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
        if (result == LayoutPreparationStatus::Failed)
        {
            beginResult_ = semantic ? semantic_.Result() : v0_.Result();
            return result;
        }
        beginResult_       = S_OK;
        requestedSemantic_ = semantic;
        if (semantic)
            v0_.CancelPreparation();
        else
            semantic_.Cancel();
        if (result == LayoutPreparationStatus::Complete)
            Publish();
        return result;
    }
    HRESULT Result() const noexcept
    {
        return beginResult_;
    }
    LayoutAdvance AdvancePreparation(LayoutWorkBudget budget = {})
    {
        auto result = requestedSemantic_ ? semantic_.Advance(budget) : v0_.AdvancePreparation(budget);
        if (result.status == LayoutPreparationStatus::Complete)
            Publish();
        return result;
    }
    std::span<const std::size_t> RejectedNativeObjects() const noexcept
    {
        return requestedSemantic_ ? semantic_.RejectedNativeObjects() : std::span<const std::size_t>{};
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
    float Width() const noexcept
    {
        return activeSemantic_ ? semantic_.Width() : v0_.Width();
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
        return activeSemantic_ && semantic_.Ready()
                   ? LayoutPreparation{semantic_.BoxCount() - semantic_.ReusedBoxCount(), semantic_.ReusedBoxCount(), SourceLength(), 0u, 0u, 0u}
                   : v0_.LastPreparation();
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
    [[nodiscard]] HRESULT TryHitTest(float x, float y, Hit& output) const
    {
        return activeSemantic_ ? semantic_.TryHitTest(x, y, output) : v0_.TryHitTest(x, y, output);
    }
    CaretGeometry Caret(std::size_t display) const
    {
        return activeSemantic_ ? semantic_.Caret(display) : v0_.Caret(display);
    }
    [[nodiscard]] HRESULT TryCaret(std::size_t display, CaretGeometry& output) const
    {
        return activeSemantic_ ? semantic_.TryCaret(display, output) : v0_.TryCaret(display, output);
    }
    CaretGeometry SourceCaret(std::size_t source, std::size_t site = MarkdownNoNode) const
    {
        return activeSemantic_ ? semantic_.SourceCaret(source, site) : v0_.Caret(v0_.ToDisplay(source));
    }
    [[nodiscard]] HRESULT TrySourceCaret(std::size_t source, std::size_t site, CaretGeometry& output) const
    {
        if (activeSemantic_)
            return semantic_.TrySourceCaret(source, site, output);
        std::size_t display{};
        if (const auto status = v0_.TryToDisplay(source, display); FAILED(status))
            return status;
        return v0_.TryCaret(display, output);
    }
    SourceCaretHit HitTestSource(float x, float y) const
    {
        if (activeSemantic_)
            return semantic_.HitTestSource(x, y);
        const auto hit = v0_.HitTest(x, y);
        return {v0_.ToSource(hit.position, hit.trailing)};
    }
    [[nodiscard]] HRESULT TryHitTestSource(float x, float y, SourceCaretHit& output) const
    {
        if (activeSemantic_)
            return semantic_.TryHitTestSource(x, y, output);
        Hit hit{};
        if (const auto status = v0_.TryHitTest(x, y, hit); FAILED(status))
            return status;
        std::size_t source{};
        if (const auto status = v0_.TryToSource(hit.position, hit.trailing, source); FAILED(status))
            return status;
        output = {source};
        return S_OK;
    }
    std::pair<std::size_t, std::size_t> VisibleSourceRange(float top, float bottom) const
    {
        if (activeSemantic_)
            return semantic_.VisibleSourceRange(top, bottom);
        const auto range = v0_.VisibleRange(top, bottom);
        return {v0_.ToSource(range.first), v0_.ToSource(range.second, true)};
    }
    [[nodiscard]] HRESULT TryVisibleSourceRange(float top, float bottom, std::pair<std::size_t, std::size_t>& output) const
    {
        if (activeSemantic_)
            return semantic_.TryVisibleSourceRange(top, bottom, output);
        std::pair<std::size_t, std::size_t> range{}, result{};
        if (const auto status = v0_.TryVisibleRange(top, bottom, range); FAILED(status))
            return status;
        if (const auto status = v0_.TryToSource(range.first, false, result.first); FAILED(status))
            return status;
        if (const auto status = v0_.TryToSource(range.second, true, result.second); FAILED(status))
            return status;
        output = result;
        return S_OK;
    }
    std::vector<D2D1_RECT_F> Range(std::size_t first, std::size_t last) const
    {
        return activeSemantic_ ? semantic_.Range(first, last) : v0_.Range(first, last);
    }
    [[nodiscard]] HRESULT TryRange(std::size_t first, std::size_t last, std::vector<D2D1_RECT_F>& output) const
    {
        return activeSemantic_ ? semantic_.TryRange(first, last, output) : v0_.TryRange(first, last, output);
    }
    std::vector<D2D1_RECT_F> SourceRange(std::size_t first, std::size_t last) const
    {
        return activeSemantic_ ? semantic_.SourceRange({first, last}) : v0_.Range(v0_.ToDisplay(first), v0_.ToDisplay(last));
    }
    [[nodiscard]] HRESULT TrySourceRange(std::size_t first, std::size_t last, std::vector<D2D1_RECT_F>& output) const
    {
        if (activeSemantic_)
            return semantic_.TrySourceRange({first, last}, output);
        if (first > last)
            return E_INVALIDARG;
        std::size_t displayFirst{}, displayLast{};
        if (const auto status = v0_.TryToDisplay(first, displayFirst); FAILED(status))
            return status;
        if (const auto status = v0_.TryToDisplay(last, displayLast); FAILED(status))
            return status;
        return v0_.TryRange(displayFirst, displayLast, output);
    }
    std::size_t PreviousCluster(std::size_t display) const
    {
        return activeSemantic_ ? semantic_.PreviousCluster(display) : v0_.PreviousCluster(display);
    }
    [[nodiscard]] HRESULT TryPreviousCluster(std::size_t display, std::size_t& output) const
    {
        return activeSemantic_ ? semantic_.TryPreviousCluster(display, output) : v0_.TryPreviousCluster(display, output);
    }
    std::size_t NextCluster(std::size_t display) const
    {
        return activeSemantic_ ? semantic_.NextCluster(display) : v0_.NextCluster(display);
    }
    [[nodiscard]] HRESULT TryNextCluster(std::size_t display, std::size_t& output) const
    {
        return activeSemantic_ ? semantic_.TryNextCluster(display, output) : v0_.TryNextCluster(display, output);
    }
    std::pair<std::size_t, std::size_t> DeletionRange(std::size_t display, bool backwards) const
    {
        return activeSemantic_ ? semantic_.DeletionRange(display, backwards) : v0_.DeletionRange(display, backwards);
    }
    [[nodiscard]] HRESULT TryDeletionRange(std::size_t display, bool backwards, std::pair<std::size_t, std::size_t>& output) const
    {
        return activeSemantic_ ? semantic_.TryDeletionRange(display, backwards, output) : v0_.TryDeletionRange(display, backwards, output);
    }
    std::size_t LineBoundary(std::size_t display, bool end) const
    {
        return activeSemantic_ ? semantic_.LineBoundary(display, end) : v0_.LineBoundary(display, end);
    }
    [[nodiscard]] HRESULT TryLineBoundary(std::size_t display, bool end, std::size_t& output) const
    {
        return activeSemantic_ ? semantic_.TryLineBoundary(display, end, output) : v0_.TryLineBoundary(display, end, output);
    }
    std::pair<std::size_t, std::size_t> VisibleRange(float top, float bottom) const
    {
        return activeSemantic_ ? semantic_.VisibleRange(top, bottom) : v0_.VisibleRange(top, bottom);
    }
    [[nodiscard]] HRESULT TryVisibleRange(float top, float bottom, std::pair<std::size_t, std::size_t>& output) const
    {
        return activeSemantic_ ? semantic_.TryVisibleRange(top, bottom, output) : v0_.TryVisibleRange(top, bottom, output);
    }
    void Draw(ID2D1RenderTarget* target,
              ID2D1Brush* brush,
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
              ID2D1Brush* diagramLabelFill             = nullptr) const
    {
#if DXUI_ENABLE_DIAGNOSTICS
        ++testRichLayoutDrawCalls;
#endif
        if (activeSemantic_)
            semantic_.Draw(target,
                           brush,
                           decoration,
                           origin,
                           top,
                           bottom,
                           fill,
                           imageOpacity,
                           links,
                           quoteFill,
                           diagramColors,
                           calloutAccent,
                           propertiesInk,
                           diagramContentInk,
                           diagramLabelFill);
        else
            v0_.Draw(target, brush, origin, top, bottom, links);
    }
    [[nodiscard]] HRESULT TryDraw(ID2D1RenderTarget* target,
                                  ID2D1Brush* brush,
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
                                  ID2D1Brush* diagramLabelFill             = nullptr) const
    {
#if DXUI_ENABLE_DIAGNOSTICS
        ++testRichLayoutDrawCalls;
#endif
        return activeSemantic_ ? semantic_.TryDraw(target,
                                                   brush,
                                                   decoration,
                                                   origin,
                                                   top,
                                                   bottom,
                                                   fill,
                                                   imageOpacity,
                                                   links,
                                                   quoteFill,
                                                   diagramColors,
                                                   calloutAccent,
                                                   propertiesInk,
                                                   diagramContentInk,
                                                   diagramLabelFill)
                               : v0_.TryDraw(target, brush, origin, top, bottom, links);
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
    HRESULT beginResult_ = S_OK;
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
