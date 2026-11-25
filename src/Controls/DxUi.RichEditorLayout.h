#pragma once
#include "DxUi.RichSemanticGeometry.h"
#include "Support/WorkCounters.h"
#include <optional>
#include <string_view>

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
        const auto result   = semantic ? _semantic.Begin(factory, std::move(document), options, drawing, epochs, std::move(images), raster)
                                       : _v0.BeginPreparation(factory, std::move(source), revision, mode, options);
        if (result == LayoutPreparationStatus::Failed)
        {
            _beginResult = semantic ? _semantic.Result() : _v0.Result();
            return result;
        }
        _beginResult       = S_OK;
        _requestedSemantic = semantic;
        if (semantic)
            _v0.CancelPreparation();
        else
            _semantic.Cancel();
        if (result == LayoutPreparationStatus::Complete)
            Publish();
        return result;
    }
    HRESULT Result() const noexcept
    {
        return _beginResult;
    }
    LayoutAdvance AdvancePreparation(LayoutWorkBudget budget = {})
    {
        auto result = _requestedSemantic ? _semantic.Advance(budget) : _v0.AdvancePreparation(budget);
        if (result.status == LayoutPreparationStatus::Complete)
            Publish();
        return result;
    }
    std::span<const NativeObjectRejection> RejectedNativeObjects() const noexcept
    {
        return _requestedSemantic ? _semantic.RejectedNativeObjects() : std::span<const NativeObjectRejection>{};
    }
    void CancelPreparation() noexcept
    {
        _v0.CancelPreparation();
        _semantic.Cancel();
    }
    void Invalidate() noexcept
    {
        _v0.Invalidate();
        _semantic.Invalidate();
    }
    void Clear() noexcept
    {
        _v0.Clear();
        _semantic.Clear();
        _activeSemantic = _requestedSemantic = false;
        _hasPublication                      = false;
        ++_generation;
    }
    bool Ready() const noexcept
    {
        return _activeSemantic ? _semantic.Ready() : _v0.Ready();
    }
    bool HasPendingPreparation() const noexcept
    {
        return _requestedSemantic ? _semantic.Pending() : _v0.HasPendingPreparation();
    }
    std::uint64_t Revision() const noexcept
    {
        return _activeSemantic ? _semantic.Revision() : _v0.Revision();
    }
    std::uint64_t Generation() const noexcept
    {
        return _generation;
    }
    float Height() const noexcept
    {
        return _activeSemantic ? _semantic.Height() : _v0.Height();
    }
    float Width() const noexcept
    {
        return _activeSemantic ? _semantic.Width() : _v0.Width();
    }
    std::size_t SourceLength() const
    {
        return _activeSemantic ? _semantic.Document().Map().Source().size() : _v0.SourceLength();
    }
    std::size_t DisplayLength() const
    {
        return _activeSemantic ? _semantic.Document().Map().Display().size() : _v0.DisplayLength();
    }
    wchar_t DisplayUnit(std::size_t index) const
    {
        return _activeSemantic ? _semantic.Document().Map().Display().at(index) : _v0.DisplayUnit(index);
    }
    bool SourceMatches(std::wstring_view source) const
    {
        return _activeSemantic ? _semantic.Document().Map().Source() == source : _v0.SourceMatches(source);
    }
    std::size_t ToDisplay(std::size_t source) const
    {
        return _activeSemantic ? _semantic.Document().Map().ToDisplay(source) : _v0.ToDisplay(source);
    }
    [[nodiscard]] HRESULT TryToDisplay(std::size_t source, std::size_t& output) const
    {
        if (! _activeSemantic)
            return _v0.TryToDisplay(source, output);
        return _semantic.Document().Map().TryToDisplay(source, MappingAffinity::Leading, output) == SourceResult::Success ? S_OK : E_INVALIDARG;
    }
    std::size_t ToSource(std::size_t display, bool trailing = false) const
    {
        return _activeSemantic ? _semantic.Document().Map().ToSource(display, trailing ? MappingAffinity::Trailing : MappingAffinity::Leading)
                               : _v0.ToSource(display, trailing);
    }
    std::size_t SourceBoundary(std::size_t display) const
    {
        return _activeSemantic ? _semantic.Document().Map().ToSource(display) : _v0.SourceBoundary(display);
    }
    std::wstring DisplayText() const
    {
        if (! _activeSemantic)
            return _v0.DisplayText();
        const auto display = _semantic.Document().Map().Display();
        auto& work         = ::DxUi::Detail::Work().rich;
        work.documentBytesCopied += display.size() * sizeof(wchar_t);
        if (! display.empty())
            ++work.documentFlattens;
        return std::wstring(display);
    }
    // The prepared display text without a copy when the semantic map holds it contiguously (valid while this layout is
    // unchanged); empty for the block-based v0 projection, which callers read through DisplayUnit.
    std::optional<std::wstring_view> ContiguousDisplayText() const
    {
        return _activeSemantic ? std::optional<std::wstring_view>(_semantic.Document().Map().Display()) : std::nullopt;
    }
    std::size_t ChunkCount() const noexcept
    {
        return _activeSemantic ? _semantic.BoxCount() : _v0.ChunkCount();
    }
    LayoutPreparation LastPreparation() const noexcept
    {
        return _activeSemantic && _semantic.Ready()
                   ? LayoutPreparation{_semantic.BoxCount() - _semantic.ReusedBoxCount(), _semantic.ReusedBoxCount(), SourceLength(), 0u, 0u, 0u}
                   : _v0.LastPreparation();
    }
    bool UsesWholeGeometry() const noexcept
    {
        return ! _activeSemantic && _v0.UsesWholeGeometry();
    }
    bool UsesWholeRaster() const noexcept
    {
        return ! _activeSemantic && _v0.UsesWholeRaster();
    }
    Hit HitTest(float x, float y) const
    {
        return _activeSemantic ? _semantic.HitTest(x, y) : _v0.HitTest(x, y);
    }
    [[nodiscard]] HRESULT TryHitTest(float x, float y, Hit& output) const
    {
        return _activeSemantic ? _semantic.TryHitTest(x, y, output) : _v0.TryHitTest(x, y, output);
    }
    CaretGeometry Caret(std::size_t display) const
    {
        return _activeSemantic ? _semantic.Caret(display) : _v0.Caret(display);
    }
    [[nodiscard]] HRESULT TryCaret(std::size_t display, CaretGeometry& output) const
    {
        return _activeSemantic ? _semantic.TryCaret(display, output) : _v0.TryCaret(display, output);
    }
    CaretGeometry SourceCaret(std::size_t source, std::size_t site = MarkdownNoNode) const
    {
        return _activeSemantic ? _semantic.SourceCaret(source, site) : _v0.Caret(_v0.ToDisplay(source));
    }
    [[nodiscard]] HRESULT TrySourceCaret(std::size_t source, std::size_t site, CaretGeometry& output) const
    {
        return TrySourceCaret(source, site, CaretAffinity::Downstream, output);
    }
    // Soft-wrap affinity applies to the ordinary layout; semantic geometry keeps
    // its own source-site affinity and ignores it.
    CaretGeometry Caret(std::size_t display, CaretAffinity affinity) const
    {
        return _activeSemantic ? _semantic.Caret(display) : _v0.Caret(display, affinity);
    }
    CaretGeometry SourceCaret(std::size_t source, std::size_t site, CaretAffinity affinity) const
    {
        return _activeSemantic ? _semantic.SourceCaret(source, site) : _v0.Caret(_v0.ToDisplay(source), affinity);
    }
    [[nodiscard]] HRESULT TrySourceCaret(std::size_t source, std::size_t site, CaretAffinity affinity, CaretGeometry& output) const
    {
        if (_activeSemantic)
            return _semantic.TrySourceCaret(source, site, output);
        std::size_t display{};
        if (const auto status = _v0.TryToDisplay(source, display); FAILED(status))
            return status;
        return _v0.TryCaret(display, affinity, output);
    }
    SourceCaretHit HitTestSource(float x, float y) const
    {
        CaretAffinity affinity{};
        return HitTestSource(x, y, affinity);
    }
    // A hit on a cluster's trailing edge in the ordinary layout reports Upstream,
    // so a click past a wrapped line's end keeps the caret on that line.
    SourceCaretHit HitTestSource(float x, float y, CaretAffinity& affinity) const
    {
        affinity = CaretAffinity::Downstream;
        if (_activeSemantic)
            return _semantic.HitTestSource(x, y);
        const auto hit = _v0.HitTest(x, y);
        if (hit.trailing)
            affinity = CaretAffinity::Upstream;
        return {_v0.ToSource(hit.position, hit.trailing)};
    }
    [[nodiscard]] HRESULT TryHitTestSource(float x, float y, SourceCaretHit& output) const
    {
        if (_activeSemantic)
            return _semantic.TryHitTestSource(x, y, output);
        Hit hit{};
        if (const auto status = _v0.TryHitTest(x, y, hit); FAILED(status))
            return status;
        std::size_t source{};
        if (const auto status = _v0.TryToSource(hit.position, hit.trailing, source); FAILED(status))
            return status;
        output = {source};
        return S_OK;
    }
    std::pair<std::size_t, std::size_t> VisibleSourceRange(float top, float bottom) const
    {
        if (_activeSemantic)
            return _semantic.VisibleSourceRange(top, bottom);
        const auto range = _v0.VisibleRange(top, bottom);
        return {_v0.ToSource(range.first), _v0.ToSource(range.second, true)};
    }
    [[nodiscard]] HRESULT TryVisibleSourceRange(float top, float bottom, std::pair<std::size_t, std::size_t>& output) const
    {
        if (_activeSemantic)
            return _semantic.TryVisibleSourceRange(top, bottom, output);
        std::pair<std::size_t, std::size_t> range{}, result{};
        if (const auto status = _v0.TryVisibleRange(top, bottom, range); FAILED(status))
            return status;
        if (const auto status = _v0.TryToSource(range.first, false, result.first); FAILED(status))
            return status;
        if (const auto status = _v0.TryToSource(range.second, true, result.second); FAILED(status))
            return status;
        output = result;
        return S_OK;
    }
    std::vector<D2D1_RECT_F> Range(std::size_t first, std::size_t last) const
    {
        return _activeSemantic ? _semantic.Range(first, last) : _v0.Range(first, last);
    }
    [[nodiscard]] HRESULT TryRange(std::size_t first, std::size_t last, std::vector<D2D1_RECT_F>& output) const
    {
        return _activeSemantic ? _semantic.TryRange(first, last, output) : _v0.TryRange(first, last, output);
    }
    std::vector<D2D1_RECT_F> SourceRange(std::size_t first, std::size_t last) const
    {
        return _activeSemantic ? _semantic.SourceRange({first, last}) : _v0.Range(_v0.ToDisplay(first), _v0.ToDisplay(last));
    }
    [[nodiscard]] HRESULT TrySourceRange(std::size_t first, std::size_t last, std::vector<D2D1_RECT_F>& output) const
    {
        if (_activeSemantic)
            return _semantic.TrySourceRange({first, last}, output);
        if (first > last)
            return E_INVALIDARG;
        std::size_t displayFirst{}, displayLast{};
        if (const auto status = _v0.TryToDisplay(first, displayFirst); FAILED(status))
            return status;
        if (const auto status = _v0.TryToDisplay(last, displayLast); FAILED(status))
            return status;
        return _v0.TryRange(displayFirst, displayLast, output);
    }
    std::size_t PreviousCluster(std::size_t display) const
    {
        return _activeSemantic ? _semantic.PreviousCluster(display) : _v0.PreviousCluster(display);
    }
    [[nodiscard]] HRESULT TryPreviousCluster(std::size_t display, std::size_t& output) const
    {
        return _activeSemantic ? _semantic.TryPreviousCluster(display, output) : _v0.TryPreviousCluster(display, output);
    }
    std::size_t NextCluster(std::size_t display) const
    {
        return _activeSemantic ? _semantic.NextCluster(display) : _v0.NextCluster(display);
    }
    [[nodiscard]] HRESULT TryNextCluster(std::size_t display, std::size_t& output) const
    {
        return _activeSemantic ? _semantic.TryNextCluster(display, output) : _v0.TryNextCluster(display, output);
    }
    std::pair<std::size_t, std::size_t> DeletionRange(std::size_t display, bool backwards) const
    {
        return _activeSemantic ? _semantic.DeletionRange(display, backwards) : _v0.DeletionRange(display, backwards);
    }
    [[nodiscard]] HRESULT TryDeletionRange(std::size_t display, bool backwards, std::pair<std::size_t, std::size_t>& output) const
    {
        return _activeSemantic ? _semantic.TryDeletionRange(display, backwards, output) : _v0.TryDeletionRange(display, backwards, output);
    }
    std::size_t LineBoundary(std::size_t display, bool end) const
    {
        return _activeSemantic ? _semantic.LineBoundary(display, end) : _v0.LineBoundary(display, end);
    }
    [[nodiscard]] HRESULT TryLineBoundary(std::size_t display, bool end, std::size_t& output) const
    {
        return _activeSemantic ? _semantic.TryLineBoundary(display, end, output) : _v0.TryLineBoundary(display, end, output);
    }
    std::pair<std::size_t, std::size_t> VisibleRange(float top, float bottom) const
    {
        return _activeSemantic ? _semantic.VisibleRange(top, bottom) : _v0.VisibleRange(top, bottom);
    }
    [[nodiscard]] HRESULT TryVisibleRange(float top, float bottom, std::pair<std::size_t, std::size_t>& output) const
    {
        return _activeSemantic ? _semantic.TryVisibleRange(top, bottom, output) : _v0.TryVisibleRange(top, bottom, output);
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
        if (_activeSemantic)
            _semantic.Draw(target,
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
            _v0.Draw(target, brush, origin, top, bottom, links);
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
        return _activeSemantic ? _semantic.TryDraw(target,
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
                               : _v0.TryDraw(target, brush, origin, top, bottom, links);
    }
    bool Semantic() const noexcept
    {
        return _activeSemantic;
    }
    const PreparedSemanticGeometry* SemanticGeometry() const noexcept
    {
        return _activeSemantic && _semantic.Ready() ? &_semantic : nullptr;
    }

private:
    HRESULT _beginResult = S_OK;
    void Publish() noexcept
    {
        const auto targetGeneration = _requestedSemantic ? _semantic.Generation() : _v0.Generation();
        if (! _hasPublication || _activeSemantic != _requestedSemantic || targetGeneration != _targetGeneration)
            ++_generation;
        _hasPublication   = true;
        _activeSemantic   = _requestedSemantic;
        _targetGeneration = targetGeneration;
        if (_activeSemantic)
            _v0.Clear();
        else
            _semantic.Clear();
    }
    PreparedDocumentLayout _v0;
    PreparedSemanticGeometry _semantic;
    std::uint64_t _generation{}, _targetGeneration{};
    bool _activeSemantic{}, _requestedSemantic{}, _hasPublication{};
};
#pragma warning(pop)
} // namespace DxUi::RichText::Detail
